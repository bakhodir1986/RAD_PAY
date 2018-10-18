using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_monitoring_cabinetDataManager
    {
        //card_monitoring_cabinetViewModel
        //public long       id              { get; set; }
        //public long       card_id         { get; set; }
        //public int?       monitoring_flag { get; set; }
        //public DateTime?  add_ts          { get; set; }
        //public DateTime?  start_date      { get; set; }
        //public DateTime?  end_date        { get; set; }
        //public long?      purchase_id     { get; set; }
        //public int?       status          { get; set; }
        //public DateTime?  off_ts          { get; set; }
        //public long?      periodic_id     { get; set; }
        //public long?      uid             { get; set; }

        public static void Add(card_monitoring_cabinetViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new card_monitoring_cabinet
            {
                id              = model.id              ,    
                card_id         = model.card_id         ,
                monitoring_flag = model.monitoring_flag ,
                add_ts          = model.add_ts          ,
                start_date      = model.start_date      ,
                end_date        = model.end_date        ,
                purchase_id     = model.purchase_id     ,
                status          = model.status          ,
                off_ts          = model.off_ts          ,
                periodic_id     = model.periodic_id     ,
                uid             = model.uid             ,
            };

            db.card_monitoring_cabinet.Add(dbmodel);
        }

        public static void Modify(card_monitoring_cabinetViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_monitoring_cabinet.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                           ;    
                    dbmodel.card_id = model.card_id                 ;
                    dbmodel.monitoring_flag = model.monitoring_flag ;
                    dbmodel.add_ts = model.add_ts                   ;
                    dbmodel.start_date = model.start_date           ;
                    dbmodel.end_date = model.end_date               ;
                    dbmodel.purchase_id = model.purchase_id         ;
                    dbmodel.status = model.status                   ;
                    dbmodel.off_ts = model.off_ts                   ;
                    dbmodel.periodic_id = model.periodic_id         ;
                    dbmodel.uid = model.uid;
                }
            }
        }

        public static void Delete(card_monitoring_cabinetViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_monitoring_cabinet.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.card_monitoring_cabinet.Remove(dbmodel);
                }
            }
        }

        public static List<card_monitoring_cabinetViewModel> Get(card_monitoring_cabinetViewModel model, RAD_PAYEntities db)
        {
            List<card_monitoring_cabinetViewModel> list = null;

            var query = from resmodel in db.card_monitoring_cabinet
                        select new card_monitoring_cabinetViewModel
                        {
                            id = resmodel.id,
                            card_id = resmodel.card_id,
                            monitoring_flag = resmodel.monitoring_flag,
                            add_ts = resmodel.add_ts,
                            start_date = resmodel.start_date,
                            end_date = resmodel.end_date,
                            purchase_id = resmodel.purchase_id,
                            status = resmodel.status,
                            off_ts = resmodel.off_ts,
                            periodic_id = resmodel.periodic_id,
                            uid = resmodel.uid,
                        };

            list = query.ToList();

            return list;
        }
    }
}