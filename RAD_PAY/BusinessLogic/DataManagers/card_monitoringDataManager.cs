using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_monitoringDataManager
    {
        //card_monitoringViewModel
        //public long       id                  { get; set; }
        //public long       card_id             { get; set; }
        //public long?      uid                 { get; set; }
        //public string     pan                 { get; set; }
        //public DateTime?  ts                  { get; set; }
        //public long?      amount              { get; set; }
        //public bool?      reversal            { get; set; }
        //public bool?      credit              { get; set; }
        //public long       refnum              { get; set; }
        //public int?       status              { get; set; }
        //public long?      rad_pid             { get; set; }
        //public long?      rad_tid             { get; set; }
        //public string     merchant_name       { get; set; }
        //public string     epos_merchant_id    { get; set; }
        //public string     epos_terminal_id    { get; set; }
        //public string     street              { get; set; }
        //public string     city                { get; set; }

        public static void Add(card_monitoringViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new card_monitoring
            {
                id              = model.id                  ,         
                card_id         = model.card_id             ,
                uid             = model.uid                 ,
                pan             = model.pan                 ,
                ts              = model.ts                  ,
                amount          = model.amount              ,
                reversal        = model.reversal            ,
                credit          = model.credit              ,
                refnum          = model.refnum              ,
                status          = model.status              ,
                rad_pid         = model.rad_pid             ,
                rad_tid         = model.rad_tid             ,
                merchant_name   = model.merchant_name       ,
                epos_merchant_id= model.epos_merchant_id    ,
                epos_terminal_id= model.epos_terminal_id    ,
                street          = model.street              ,
                city            = model.city                ,
            };

            db.card_monitoring.Add(dbmodel);
        }

        public static void Modify(card_monitoringViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_monitoring.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                               ;        
                    dbmodel.card_id = model.card_id                     ;
                    dbmodel.uid = model.uid                             ;
                    dbmodel.pan = model.pan                             ;
                    dbmodel.ts = model.ts                               ;
                    dbmodel.amount = model.amount                       ;
                    dbmodel.reversal = model.reversal                   ;
                    dbmodel.credit = model.credit                       ;
                    dbmodel.refnum = model.refnum                       ;
                    dbmodel.status = model.status                       ;
                    dbmodel.rad_pid = model.rad_pid                     ;
                    dbmodel.rad_tid = model.rad_tid                     ;
                    dbmodel.merchant_name = model.merchant_name         ;
                    dbmodel.epos_merchant_id = model.epos_merchant_id   ;
                    dbmodel.epos_terminal_id = model.epos_terminal_id   ;
                    dbmodel.street = model.street                       ;
                    dbmodel.city = model.city;
                }
            }
        }

        public static void Delete(card_monitoringViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_monitoring.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.card_monitoring.Remove(dbmodel);
                }
            }
        }

        public static List<card_monitoringViewModel> Get(card_monitoringViewModel model, RAD_PAYEntities db)
        {
            List<card_monitoringViewModel> list = null;

            var query = from resmodel in db.card_monitoring
                        select new card_monitoringViewModel
                        {
                            id = resmodel.id,
                            card_id = resmodel.card_id,
                            uid = resmodel.uid,
                            pan = resmodel.pan,
                            ts = resmodel.ts,
                            amount = resmodel.amount,
                            reversal = resmodel.reversal,
                            credit = resmodel.credit,
                            refnum = resmodel.refnum,
                            status = resmodel.status,
                            rad_pid = resmodel.rad_pid,
                            rad_tid = resmodel.rad_tid,
                            merchant_name = resmodel.merchant_name,
                            epos_merchant_id = resmodel.epos_merchant_id,
                            epos_terminal_id = resmodel.epos_terminal_id,
                            street = resmodel.street,
                            city = resmodel.city,
                        };

            list = query.ToList();

            return list;
        }
    }
}