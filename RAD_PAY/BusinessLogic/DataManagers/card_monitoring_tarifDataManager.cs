using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_monitoring_tarifDataManager
    {
        //card_monitoring_tarifViewModel
        //public long       id      { get; set; }
        //public long?      amount  { get; set; }
        //public int?       mid     { get; set; }
        //public int?       status  { get; set; }

        public static void Add(card_monitoring_tarifViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new card_monitoring_tarif
            {
                id      = model.id      ,
                amount  = model.amount  ,
                mid     = model.mid     ,
                status  = model.status  ,
            };

            db.card_monitoring_tarif.Add(dbmodel);
        }

        public static void Modify(card_monitoring_tarifViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_monitoring_tarif.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id           ;
                    dbmodel.amount = model.amount   ;
                    dbmodel.mid = model.mid         ;
                    dbmodel.status = model.status   ;
                }
            }
        }

        public static void Delete(card_monitoring_tarifViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_monitoring_tarif.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.card_monitoring_tarif.Remove(dbmodel);
                }
            }
        }

        public static List<card_monitoring_tarifViewModel> Get(card_monitoring_tarifViewModel model, RAD_PAYEntities db)
        {
            List<card_monitoring_tarifViewModel> list = null;

            var query = from resmodel in db.card_monitoring_tarif
                        select new card_monitoring_tarifViewModel
                        {
                            id = resmodel.id,
                            amount = resmodel.amount,
                            mid = resmodel.mid,
                            status = resmodel.status,
                        };

            list = query.ToList();

            return list;
        }
    }
}