using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class bank_bonusDataManager
    {
        //bank_bonusViewModel
        //public long       id              { get; set; }
        //public long       bank_id         { get; set; }
        //public long?      min_amount      { get; set; }
        //public int?       percent         { get; set; }
        //public DateTime?  start_date      { get; set; }
        //public DateTime?  end_date        { get; set; }
        //public int?       status          { get; set; }
        //public double?    longitude       { get; set; }
        //public double?    latitude        { get; set; }
        //public string     description     { get; set; }
        //public long?      bonus_amount    { get; set; }

        public static void Add(bank_bonusViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new bank_bonus
            {
                id           = model.id           ,      
                bank_id      = model.bank_id      ,
                min_amount   = model.min_amount   ,
                percent      = model.percent      ,
                start_date   = model.start_date   ,
                end_date     = model.end_date     ,
                status       = model.status       ,
                longitude    = model.longitude    ,
                latitude     = model.latitude     ,
                description  = model.description  ,
                bonus_amount = model.bonus_amount ,
            };

            db.bank_bonus.Add(dbmodel);
        }

        public static void Modify(bank_bonusViewModel model, RAD_PAYEntities db)
        {
            var result = db.bank_bonus.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                       ;       
                    dbmodel.bank_id = model.bank_id             ;
                    dbmodel.min_amount = model.min_amount       ;
                    dbmodel.percent = model.percent             ;
                    dbmodel.start_date = model.start_date       ;
                    dbmodel.end_date = model.end_date           ;
                    dbmodel.status = model.status               ;
                    dbmodel.longitude = model.longitude         ;
                    dbmodel.latitude = model.latitude           ;
                    dbmodel.description = model.description     ;
                    dbmodel.bonus_amount = model.bonus_amount   ;
                }
            }
        }

        public static void Delete(bank_bonusViewModel model, RAD_PAYEntities db)
        {
            var result = db.bank_bonus.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.bank_bonus.Remove(dbmodel);
                }
            }
        }

        public static List<bank_bonusViewModel> Get(bank_bonusViewModel model, RAD_PAYEntities db)
        {
            List<bank_bonusViewModel> list = null;

            var query = from resmodel in db.bank_bonus
                        select new bank_bonusViewModel
                        {
                            id = resmodel.id,
                            bank_id = resmodel.bank_id,
                            min_amount = resmodel.min_amount,
                            percent = resmodel.percent,
                            start_date = resmodel.start_date,
                            end_date = resmodel.end_date,
                            status = resmodel.status,
                            longitude = resmodel.longitude,
                            latitude = resmodel.latitude,
                            description = resmodel.description,
                            bonus_amount = resmodel.bonus_amount,
                        };

            list = query.ToList();

            return list;
        }
    }
}